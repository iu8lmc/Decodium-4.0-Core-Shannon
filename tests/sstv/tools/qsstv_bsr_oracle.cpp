// SPDX-License-Identifier: GPL-3.0-or-later
// Runs the pinned QSSTV storeBSR/processFIX method bodies in minimal shims.
#include "src/sstv/digital/HamDrmBsrCodec.h"
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

struct Segment { bool present; bool hasData() const { return present; } };
struct transportBlock {
    int totalSegments {0}, defaultSegmentSize {0}, transportID {0}, modeCode {0};
    QList<Segment*> dataSegmentPtrList;
    QByteArray baBSR;
    QString fileName;
};
struct sourceDecoder { QList<int> erasureList; bool storeBSR(transportBlock*, bool); };
struct drmTx { QList<int> fixBlockList; int processFIX(QByteArray); };
#include "qsstv_bsr_methods.inc"

int main()
{
    using namespace decodium::sstv::hamdrm;
    std::vector<std::vector<std::uint16_t>> cases {
        {1, 2, 3, 7, 10, 11}, {0, 4, 5, 9, 10, 11, 15},
        {5}, {2, 3, 4, 5}, {1, 3, 5, 7}, {0, 1, 7, 8, 14, 15}
    };
    // Exhaust patterns in each half, including alternating holes and the
    // first and last segment of the object.
    for (unsigned offset : {0U, 8U})
        for (unsigned mask = 1; mask < 256; ++mask) {
            std::vector<std::uint16_t> missing;
            for (unsigned bit = 0; bit < 8; ++bit)
                if ((mask & (1U << bit)) != 0U)
                    missing.push_back(static_cast<std::uint16_t>(offset + bit));
            cases.push_back(std::move(missing));
        }
    int checked = 0;
    for (std::size_t c = 0; c < cases.size(); ++c) {
        for (const bool compact : {true, false}) {
            transportBlock block;
            block.totalSegments = 16;
            block.defaultSegmentSize = c % 2 ? 128 : 82;
            block.transportID = c % 2 ? 0x4321 : 0x2345;
            block.modeCode = 0;
            block.fileName = c % 2 ? QStringLiteral("image-b.jpg") : QStringLiteral("image-a.jpg");
            std::vector<std::unique_ptr<Segment>> segments;
            for (int i = 0; i < block.totalSegments; ++i) {
                segments.push_back(std::make_unique<Segment>(Segment {
                    std::find(cases[c].begin(), cases[c].end(), i) == cases[c].end()}));
                block.dataSegmentPtrList.append(segments.back().get());
            }
            sourceDecoder reference;
            if (!reference.storeBSR(&block, compact)) return 1;
            HamDrmBsrRequest request;
            request.transportId = block.transportID;
            request.segmentSize = block.defaultSegmentSize;
            request.missingSegments = cases[c];
            if (!compact) {
                request.filename = block.fileName.toStdString();
                request.qsstvCompatibilityCode = 0;
            }
            const auto encoded = encodeHamDrmBsr(request, compact
                ? HamDrmBsrDialect::EasyPalCompatible : HamDrmBsrDialect::QsstvExtended);
            if (!encoded.ok()) return 2;
            QByteArray native(reinterpret_cast<const char*>(encoded.value->data()), int(encoded.value->size()));
            // QSSTV omits the optional final newline after the extended mode.
            if (!compact && native.endsWith('\n')) native.chop(1);
            if (native != block.baBSR) return 3;
            drmTx receiver;
            if (receiver.processFIX(native) != block.transportID) return 4;
            const auto decoded = parseHamDrmBsr(
                reinterpret_cast<const std::uint8_t*>(block.baBSR.constData()), std::size_t(block.baBSR.size()));
            if (!decoded.ok() || decoded.value->transportId != block.transportID
                || decoded.value->segmentSize != block.defaultSegmentSize
                || decoded.value->missingSegments.size() != std::size_t(receiver.fixBlockList.size())) return 5;
            for (qsizetype i = 0; i < receiver.fixBlockList.size(); ++i)
                if (decoded.value->missingSegments[std::size_t(i)] != receiver.fixBlockList[i]) return 6;
            if (!std::includes(receiver.fixBlockList.cbegin(), receiver.fixBlockList.cend(),
                               cases[c].begin(), cases[c].end())) return 7;
            ++checked;
        }
    }
    for (const QByteArray invalid : {QByteArray("1\nH_OK\n82\n1\n-1\n-99\n"),
                                     QByteArray("1\nH_OK\n82\n1\n-1\n0\n-99\n")}) {
        const auto rejected = parseHamDrmBsr(
            reinterpret_cast<const std::uint8_t*>(invalid.constData()), std::size_t(invalid.size()));
        if (rejected.ok()) return 8;
    }
    std::cout << "PASS: " << checked << " QSSTV/Decodium BSR cases, exact wire content and requested blocks\n";
}
