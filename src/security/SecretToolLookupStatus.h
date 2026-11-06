// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace secure_settings {

enum class SecretToolLookupStatus { Found, Missing, Failed };

// libsecret's secret-tool returns 1 with no diagnostic for a missing item;
// usage errors and other unexpected exit statuses are not evidence of absence.
constexpr SecretToolLookupStatus classifySecretToolLookup(int exitCode, bool hasDiagnostic)
{
    return exitCode == 0 ? SecretToolLookupStatus::Found
        : exitCode == 1 && !hasDiagnostic ? SecretToolLookupStatus::Missing
        : SecretToolLookupStatus::Failed;
}

} // namespace secure_settings
