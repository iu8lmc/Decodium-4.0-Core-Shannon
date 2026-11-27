// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "HamDrmTypes.h"
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm {

// The legacy hybrid representation is reversible obfuscation, not encryption.
// Every field, including the password, is disclosed to anyone receiving it.
struct HamDrmHybridServer final {
    std::string host;
    std::string user;
    std::string password;
    std::string directory;
};

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmHybridServer(const HamDrmHybridServer& server);
HamDrmValueResult<HamDrmHybridServer> decodeHamDrmHybridServer(const std::vector<std::uint8_t>& body);
}
