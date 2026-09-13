// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvModeSpec.h"

#include <string>
#include <string_view>
#include <vector>

namespace decodium::sstv {

enum class ModeValidationCode {
    EmptyId,
    EmptyLongName,
    EmptyShortName,
    EmptyFamily,
    DuplicateId,
    DuplicateLongName,
    DuplicateShortName,
    BlockedWithoutNote,
    MissingProtocolField,
    InvalidProtocolValue,
    InvalidVis,
    DuplicateVisCode,
    CapabilityWithoutProtocolData,
    CapabilityWithoutEvidence,
    VerifiedWithoutIndependentEvidence,
    InconsistentStatus
};

struct ModeValidationIssue final
{
    ModeValidationCode code;
    std::string modeId;
    std::string message;
};

// Immutable-after-construction registry.  canonical() creates an independent
// value and therefore introduces no process-global mutable mode state.
class SstvModeRegistry final
{
public:
    explicit SstvModeRegistry(std::vector<SstvModeSpec> modes);

    static SstvModeRegistry canonical();
    static std::vector<ModeValidationIssue> validate(const std::vector<SstvModeSpec>& modes);

    const std::vector<SstvModeSpec>& modes() const noexcept;
    const SstvModeSpec* findById(std::string_view id) const noexcept;
    const SstvModeSpec* findByName(std::string_view name) const noexcept;
    std::vector<ModeValidationIssue> validationIssues() const;
    bool isValid() const;

private:
    std::vector<SstvModeSpec> m_modes;
};

bool containsValidationIssue(const std::vector<ModeValidationIssue>& issues,
                             ModeValidationCode code) noexcept;

} // namespace decodium::sstv
