// SPDX-License-Identifier: GPL-3.0-or-later
// Run the unmodified TRXAMADRM 3.6 BSR segment loop on guarded storage.
#include "src/sstv/digital/HamDrmBsrCodec.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    using namespace decodium::sstv::hamdrm;
    for (const auto& missing : {std::vector<std::uint16_t>{1, 3},
                                std::vector<std::uint16_t>{1, 2, 3}}) {
        for (const auto dialect : {HamDrmBsrDialect::EasyPalCompatible,
                                   HamDrmBsrDialect::QsstvExtended}) {
            HamDrmBsrRequest req;
            req.transportId = 123;
            req.segmentSize = 82;
            req.missingSegments = missing;
            req.filename = "test.jpg";
            req.qsstvCompatibilityCode = 0;
            auto wire = encodeHamDrmBsr(req, dialect);
            if (!wire.ok()) return 1;
            FILE* fpbsrbin = std::tmpfile();
            if (!fpbsrbin) return 2;
            if (std::fwrite(wire.value->data(), 1, wire.value->size(), fpbsrbin) != wire.value->size()) return 3;
            std::rewind(fpbsrbin);
            char regel[80];
            // Skip transport id, H_OK and size, as the original reader does.
            for (int n = 0; n < 3; ++n) if (!std::fgets(regel, 80, fpbsrbin)) return 4;
            std::array<int, 18> guarded {};
            guarded.front() = 123456;
            guarded.back() = 123456;
            int* list_missing = guarded.data() + 1;
            int iNumSegStore = 16, i = 0, cnt_missing = 0, mis_item = 0;
#include "trxamadrm_bsr_loop.inc"
            std::fclose(fpbsrbin);
            const bool compactRange = dialect == HamDrmBsrDialect::EasyPalCompatible && missing.size() == 3;
            if ((guarded.front() != 123456) != compactRange || guarded.back() != 123456) return 5;
            std::vector<std::uint16_t> seen;
            for (i = 0; i < iNumSegStore; ++i) if (list_missing[i]) seen.push_back(static_cast<std::uint16_t>(i));
            if ((seen != missing) != compactRange) return 6;
            std::printf("PASS %s %s: %s\n", missing.size() == 2 ? "A gaps" : "B run",
                        dialect == HamDrmBsrDialect::EasyPalCompatible ? "compact" : "extended",
                        compactRange ? "original parser writes before its segment array" : "requested segments match");
        }
    }
    if (argc != 1 && argc != 3) return 7;
    for (int arg = 1; arg < argc; ++arg) {
        FILE* fpbsrbin = std::fopen(argv[arg], "rb");
        if (!fpbsrbin) return 8;
        char regel[80];
        for (int n = 0; n < 3; ++n) if (!std::fgets(regel, 80, fpbsrbin)) return 9;
        std::array<int, 18> guarded {};
        guarded.front() = 123456;
        guarded.back() = 123456;
        int* list_missing = guarded.data() + 1;
        int iNumSegStore = 16, i = 0, cnt_missing = 0, mis_item = 0;
#include "trxamadrm_bsr_loop.inc"
        std::fclose(fpbsrbin);
        const bool compact = arg == 2;
        if ((guarded.front() != 123456) != compact || guarded.back() != 123456) return 10;
        std::vector<int> seen;
        for (i = 0; i < iNumSegStore; ++i) if (list_missing[i]) seen.push_back(i);
        if (seen != (compact ? std::vector<int>{1, 3} : std::vector<int>{1, 2, 3})) return 11;
        std::printf("PASS app %c: %s\n", arg == 1 ? 'A' : 'B',
                    compact ? "original parser writes before its segment array" : "requested segments match");
    }
}
