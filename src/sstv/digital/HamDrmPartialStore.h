// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmObjectAssembler.h"
#include "HamDrmTypes.h"

#include <QString>

#include <cstdint>

namespace decodium::sstv::hamdrm {

// Atomic, checksum-protected persistence for validated partial MOT objects.
// Paths are derived only from the 16-bit transport ID; remote filenames never
// participate in filesystem path construction.
class HamDrmPartialStore final
{
public:
    explicit HamDrmPartialStore(QString rootDirectory,
                                HamDrmLimits limits = {});

    QString partialDirectory() const;
    QString pathForTransportId(std::uint16_t transportId) const;

    HamDrmStatus save(const HamDrmObjectAssembler& assembler) const;
    HamDrmValueResult<HamDrmObjectAssembler> load(
        std::uint16_t transportId) const;
    HamDrmStatus remove(std::uint16_t transportId) const;

private:
    QString rootDirectory_;
    HamDrmLimits limits_;
};

} // namespace decodium::sstv::hamdrm
