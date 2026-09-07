// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvModeSpec.h"

namespace decodium::sstv {
namespace {

bool isSupportClaim(CapabilityStatus status) noexcept
{
    return status == CapabilityStatus::Implemented || status == CapabilityStatus::Verified;
}

} // namespace

bool SstvModeSpec::claimsRxSupport() const noexcept
{
    return isSupportClaim(rxStatus);
}

bool SstvModeSpec::claimsTxSupport() const noexcept
{
    return isSupportClaim(txStatus);
}

bool SstvModeSpec::claimsAnySupport() const noexcept
{
    return claimsRxSupport() || claimsTxSupport();
}

bool SstvModeSpec::hasImplementationEvidence() const noexcept
{
    return evidenceStatus == EvidenceStatus::DeterministicTests
        || evidenceStatus == EvidenceStatus::IndependentVector;
}

bool SstvModeSpec::hasIndependentEvidence() const noexcept
{
    return evidenceStatus == EvidenceStatus::IndependentVector
        && interoperabilityStatus == InteroperabilityStatus::IndependentlyVerified
        && fixtureStatus == FixtureStatus::Independent;
}

} // namespace decodium::sstv
