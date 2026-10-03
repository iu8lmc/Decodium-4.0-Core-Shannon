// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/integration/SstvQsoLog.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace decodium::sstv;

namespace {

constexpr std::size_t kMaximumFuzzInputBytes =
    static_cast<std::size_t>(SstvQsoLog::kMaximumAdifBytes);

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
    const char* bytes = size == 0U
        ? "" : reinterpret_cast<const char*>(data);
    const QString record = QString::fromUtf8(
        bytes, static_cast<qsizetype>(size));
    const auto parsed = SstvQsoLog::validateGeneratedAdif(record);
    if (!parsed.ok) {
        return 0;
    }
    QString error;
    if (parsed.associationId.isEmpty()
        || SstvQsoLog::associationIdForFields(parsed.fields, &error)
               != parsed.associationId
        || parsed.fields.value(QStringLiteral("MODE")).trimmed().toUpper()
               != QStringLiteral("SSTV")
        || parsed.fields.contains(QStringLiteral("SUBMODE"))) {
        invariantFailure();
    }
    const QString call = parsed.fields.value(QStringLiteral("CALL"));
    if (!call.isEmpty()
        && SstvQsoLog::validateGeneratedAdif(record, {call}).ok) {
        invariantFailure();
    }
    return 0;
}
