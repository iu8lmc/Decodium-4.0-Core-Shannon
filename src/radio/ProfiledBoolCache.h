#pragma once

#include <QString>
#include <utility>

namespace decodium {

// Cache a value only for the active settings profile. The owning object must
// invalidate it when the value is written in the current profile.
class ProfiledBoolCache
{
public:
    template <typename ReadValue>
    bool value(const QString& profile, ReadValue&& readValue) const
    {
        if (!m_valid || m_profile != profile) {
            m_value = std::forward<ReadValue>(readValue)();
            m_profile = profile;
            m_valid = true;
        }
        return m_value;
    }

    void invalidate() { m_valid = false; }

private:
    mutable QString m_profile;
    mutable bool m_value {false};
    mutable bool m_valid {false};
};

} // namespace decodium
