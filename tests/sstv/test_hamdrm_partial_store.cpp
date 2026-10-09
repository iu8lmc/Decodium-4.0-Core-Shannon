// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmMotCodec.h"
#include "src/sstv/digital/HamDrmObjectAssembler.h"
#include "src/sstv/digital/HamDrmPartialStore.h"

#include <QFile>
#include <QTemporaryDir>

#include <cstdint>
#include <iostream>
#include <vector>

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

std::vector<std::uint8_t> payload(std::size_t size)
{
    std::vector<std::uint8_t> output(size);
    for (std::size_t index = 0U; index < size; ++index) {
        output[index] = static_cast<std::uint8_t>((index * 29U + 7U) & 0xffU);
    }
    return output;
}

} // namespace

int main()
{
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    if (!temporary.isValid()) {
        return 1;
    }

    hamdrm::HamDrmMotObjectMetadata metadata;
    metadata.transportId = 0x4567U;
    metadata.filename = "partial.jpg";
    const auto body = payload(700U);
    const auto encoded = hamdrm::encodeHamDrmObject(metadata, body, 82U);
    CHECK(encoded.ok());
    if (!encoded.ok()) {
        return 1;
    }

    hamdrm::HamDrmObjectAssembler partial(metadata.transportId);
    CHECK(partial.ingest(encoded.value->headerGroups.front().data(),
                         encoded.value->headerGroups.front().size()).ok());
    CHECK(partial.ingest(encoded.value->bodyGroups.front().data(),
                         encoded.value->bodyGroups.front().size()).ok());
    CHECK(partial.ingest(encoded.value->bodyGroups.back().data(),
                         encoded.value->bodyGroups.back().size()).ok());

    const hamdrm::HamDrmPartialStore store(temporary.path());
    CHECK(store.save(partial).ok());
    CHECK(QFile::exists(store.pathForTransportId(metadata.transportId)));
    const auto restored = store.load(metadata.transportId);
    CHECK(restored.ok());
    CHECK(restored.ok() && restored.value->progress().headerComplete);
    CHECK(restored.ok() && restored.value->progress().bodySegmentsReceived == 2U);
    CHECK(restored.ok()
          && restored.value->missingBodySegments()
              == partial.missingBodySegments());

    QFile damaged(store.pathForTransportId(metadata.transportId));
    CHECK(damaged.open(QIODevice::ReadWrite));
    if (damaged.isOpen()) {
        CHECK(damaged.seek(25));
        const QByteArray original = damaged.read(1);
        CHECK(original.size() == 1);
        CHECK(damaged.seek(25));
        const char changed = static_cast<char>(original.front() ^ 0x40);
        CHECK(damaged.write(&changed, 1) == 1);
        damaged.close();
    }
    const auto rejectedDamage = store.load(metadata.transportId);
    CHECK(!rejectedDamage.ok());
    CHECK(rejectedDamage.status.code == hamdrm::HamDrmErrorCode::CrcMismatch);

    CHECK(store.save(partial).ok());
    QFile truncated(store.pathForTransportId(metadata.transportId));
    CHECK(truncated.open(QIODevice::ReadWrite));
    if (truncated.isOpen()) {
        CHECK(truncated.resize(16));
        truncated.close();
    }
    CHECK(!store.load(metadata.transportId).ok());

    CHECK(store.save(partial).ok());
    CHECK(store.remove(metadata.transportId).ok());
    CHECK(!QFile::exists(store.pathForTransportId(metadata.transportId)));
    CHECK(store.remove(metadata.transportId).ok());

    if (failures != 0) {
        std::cerr << failures << " HAMDRM partial-store checks failed\n";
        return 1;
    }
    std::cout << "HAMDRM partial-store checks passed\n";
    return 0;
}
