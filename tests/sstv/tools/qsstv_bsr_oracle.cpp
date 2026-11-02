// SPDX-License-Identifier: GPL-3.0-or-later
// Runs the pinned QSSTV storeBSR/processFIX method bodies in minimal shims.
#include "src/sstv/digital/HamDrmBsrCodec.h"
#include <QByteArray>
#include <QFile>
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

int main(int argc, char** argv)
{
    using namespace decodium::sstv::hamdrm;
    const bool missingFinal = argc > 1 && QByteArray(argv[1]) == "--missing-final";
    for (int arg = missingFinal ? 2 : 1; arg < argc; ++arg) {
        QFile log(QString::fromLocal8Bit(argv[arg]));
        if (!log.open(QIODevice::ReadOnly)) return 9;
        const QByteArray content = log.readAll();
        const QByteArray marker("[LAB][REPAIR] received BSR ");
        const qsizetype start = content.indexOf(marker);
        if (start < 0 || content.indexOf(marker, start + marker.size()) >= 0) return 10;
        const qsizetype bodyStart = start + marker.size();
        const qsizetype bodyEnd = content.indexOf("\n\n", bodyStart);
        if (bodyEnd < 0) return 11;
        QByteArray observed = content.mid(bodyStart, bodyEnd - bodyStart) + '\n';
        const auto decoded = parseHamDrmBsr(
            reinterpret_cast<const std::uint8_t*>(observed.constData()), std::size_t(observed.size()));
        const std::vector<std::uint16_t> expected = missingFinal
            ? std::vector<std::uint16_t>({2}) : std::vector<std::uint16_t>({0, 1});
        if (!decoded.ok() || decoded.value->segmentSize != 1024U
            || decoded.value->missingSegments != expected) return 12;
        drmTx receiver;
        if (receiver.processFIX(observed) != decoded.value->transportId
            || receiver.fixBlockList != (missingFinal ? QList<int>({2}) : QList<int>({0, 1}))) return 13;
        transportBlock block;
        block.totalSegments = 3;
        block.defaultSegmentSize = 1024;
        block.transportID = decoded.value->transportId;
        block.modeCode = decoded.value->qsstvCompatibilityCode.value_or(0);
        if (decoded.value->filename) block.fileName = QString::fromStdString(*decoded.value->filename);
        Segment first {missingFinal}, second {missingFinal}, last {!missingFinal};
        block.dataSegmentPtrList = {&first, &second, &last};
        sourceDecoder reference;
        if (!reference.storeBSR(&block, !decoded.value->filename.has_value())) return 14;
        if (observed.endsWith('\n')) observed.chop(1);
        QByteArray generated = block.baBSR;
        if (generated.endsWith('\n')) generated.chop(1);
        if (observed != generated) return 15;
        std::cout << "PASS observed on-audio BSR matched QSSTV methods: " << argv[arg] << '\n';
    }
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
    // Exercise long objects and ranges that cross the previously tested
    // eight-segment boundaries, including the highest default MOT segment.
    cases.insert(cases.end(), {
        {0, 7, 8, 9, 15, 16, 31, 32, 63},
        {60, 61, 62, 63, 64, 65, 127, 128, 129},
        {0, 255, 256, 257, 1022, 1023},
        {0, 32765, 32766, 32767},
        {32760, 32761, 32762, 32763, 32764, 32765, 32766, 32767}
    });
    // Deterministic A/B-like patterns across a wider object, including
    // multiple consecutive runs separated by present segments.
    std::uint32_t state = 0x539a7U;
    for (unsigned c = 0; c < 256; ++c) {
        std::vector<std::uint16_t> missing;
        for (unsigned bit = 0; bit < 128; ++bit) {
            state = state * 1664525U + 1013904223U;
            if ((state >> 29U) < 3U)
                missing.push_back(static_cast<std::uint16_t>(bit));
        }
        if (!missing.empty()) cases.push_back(std::move(missing));
    }
    int checked = 0;
    for (std::size_t c = 0; c < cases.size(); ++c) {
        for (const bool compact : {true, false}) {
            transportBlock block;
            block.totalSegments = std::max(16, int(cases[c].back()) + 1);
            block.defaultSegmentSize = c % 2 ? 128 : 82;
            block.transportID = c % 2 ? 0x4321 : 0x2345;
            block.modeCode = 0;
            block.fileName = c % 2 ? QStringLiteral("image-b.jpg") : QStringLiteral("image-a.jpg");
            std::vector<std::unique_ptr<Segment>> segments;
            for (int i = 0; i < block.totalSegments; ++i) {
                segments.push_back(std::make_unique<Segment>(Segment {
                    !std::binary_search(cases[c].begin(), cases[c].end(), i)}));
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
    for (const QByteArray wire : {
             QByteArray("\n90\r29\r\n\r\nH_\rOK\r\n82\r\n1\r\n\r\n-1\r\n3\r\n-99\r\n"),
             QByteArray("\r\n9029\nH_OK\n\n82\n1\n\n3\n-99\nimage.jpg\n00000\n")}) {
        drmTx reference;
        const int id = reference.processFIX(wire);
        const auto decoded = parseHamDrmBsr(
            reinterpret_cast<const std::uint8_t*>(wire.constData()), std::size_t(wire.size()));
        if (!decoded.ok() || id != decoded.value->transportId
            || std::size_t(reference.fixBlockList.size()) != decoded.value->missingSegments.size()) return 16;
        for (qsizetype i = 0; i < reference.fixBlockList.size(); ++i)
            if (reference.fixBlockList[i] != decoded.value->missingSegments[std::size_t(i)]) return 17;
    }
    for (const QByteArray invalid : {QByteArray("1\nH_OK\n82\n1\n-1\n-99\n"),
                                     QByteArray("1\nH_OK\n82\n1\n-1\n0\n-99\n")}) {
        const auto rejected = parseHamDrmBsr(
            reinterpret_cast<const std::uint8_t*>(invalid.constData()), std::size_t(invalid.size()));
        if (rejected.ok()) return 8;
    }
    std::cout << "PASS: " << checked << " QSSTV/Decodium BSR cases, exact wire content and requested blocks\n";
}
