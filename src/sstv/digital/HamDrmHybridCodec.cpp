// SPDX-License-Identifier: GPL-3.0-or-later
#include "HamDrmHybridCodec.h"
#include <algorithm>
#include <array>

namespace decodium::sstv::hamdrm {
namespace {
constexpr std::size_t maxPlainBytes = 1024;
bool validField(const std::string& field)
{
    return std::all_of(field.begin(), field.end(), [](unsigned char c) {
        return c >= 0x20 && c <= 0x7e && c != '?' && c != '"' && c != '<' && c != '>' && c != ':';
    });
}
bool valid(const HamDrmHybridServer& server)
{
    return !server.host.empty() && validField(server.host) && validField(server.user)
        && validField(server.password) && validField(server.directory)
        && server.host.size() + server.user.size() + server.password.size() + server.directory.size() + 5 <= maxPlainBytes;
}
void appendValue(std::vector<std::uint8_t>& out, unsigned value)
{
    auto hi = static_cast<std::uint8_t>(value >> 8);
    auto lo = static_cast<std::uint8_t>(value);
    if (value == 0) hi = lo = 0xff;
    if (hi == 0xff) hi = 0xfe;
    if (lo == 0) { lo = hi; hi = 0xfd; }
    out.push_back(hi); out.push_back(lo);
}
int hexValue(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
}

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmHybridServer(const HamDrmHybridServer& server)
{
    if (!valid(server)) return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument, "invalid hybrid server fields")};
    std::string plain = "?" + server.host + '"' + server.user + '<' + server.password + '>' + server.directory + ':';
    std::reverse(plain.begin(), plain.end());
    static constexpr char hex[] = "0123456789abcdef";
    std::vector<unsigned> digits;
    for (unsigned char c : plain) { digits.push_back(hex[c >> 4]); digits.push_back(hex[c & 15]); }
    while (digits.size() % 4) digits.push_back(0);
    std::vector<std::uint8_t> out;
    out.reserve(digits.size() * 2 + 2);
    for (std::size_t i = 0; i < digits.size(); i += 2) {
        appendValue(out, 4 * digits[i] + 7 * digits[i + 1]);
        appendValue(out, 5 * digits[i] + digits[i + 1]);
    }
    out.push_back('\r'); out.push_back('\n');
    return {std::move(out), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmHybridServer> decodeHamDrmHybridServer(const std::vector<std::uint8_t>& body)
{
    const auto fail = [] { return HamDrmValueResult<HamDrmHybridServer>{std::nullopt,
        HamDrmStatus::failure(HamDrmErrorCode::Malformed, "invalid hybrid server body")}; };
    if (body.size() < 26 || body.size() > (maxPlainBytes * 4 + 10)
        || body[body.size()-2] != '\r' || body.back() != '\n' || (body.size()-2) % 8) return fail();
    std::string reversePlain;
    bool padding = false;
    for (std::size_t i = 0; i < body.size()-2; i += 4) {
        std::array<int, 2> v{};
        for (unsigned j = 0; j < 2; ++j) {
            unsigned hi = body[i + j*2], lo = body[i + j*2 + 1];
            if (hi == 0xfe && lo == 0xff) hi = lo = 0;
            else if (hi == 0xfd) { hi = lo; lo = 0; }
            else if (hi >= 0xfd || lo == 0) return fail();
            v[j] = static_cast<int>(hi * 256 + lo);
        }
        const int n1 = v[0] - 7 * v[1], n2 = 4 * v[1] - 5 * v[0];
        if (n1 % -31 || n2 % -31) return fail();
        const int a = n1 / -31, b = n2 / -31;
        if (a == 0 && b == 0 && i + 4 == body.size()-2) { padding = true; continue; }
        const int high = hexValue(a), low = hexValue(b);
        if (padding || high < 0 || low < 0) return fail();
        reversePlain.push_back(static_cast<char>(high * 16 + low));
    }
    std::reverse(reversePlain.begin(), reversePlain.end());
    if (reversePlain.empty() || reversePlain.front() != '?' || reversePlain.back() != ':') return fail();
    const auto a = reversePlain.find('"', 1), b = reversePlain.find('<', a == std::string::npos ? 0 : a + 1);
    const auto c = reversePlain.find('>', b == std::string::npos ? 0 : b + 1);
    if (a == std::string::npos || b == std::string::npos || c == std::string::npos) return fail();
    HamDrmHybridServer server{reversePlain.substr(1, a-1), reversePlain.substr(a+1, b-a-1),
        reversePlain.substr(b+1, c-b-1), reversePlain.substr(c+1, reversePlain.size()-c-2)};
    if (!valid(server)) return fail();
    return {std::move(server), HamDrmStatus::success()};
}
}
