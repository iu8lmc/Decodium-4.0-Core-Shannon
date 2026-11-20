#ifndef DECODIUM_Q65_DECODE_CANCELLATION_HPP
#define DECODIUM_Q65_DECODE_CANCELLATION_HPP

#include <atomic>
#include <cstdint>

namespace decodium { namespace q65 {
namespace cancellation_detail {
inline thread_local std::atomic<std::uint64_t> const* retired = nullptr;
inline thread_local std::uint64_t serial = 0;
}

// Only an explicit session retirement cancels work. A newer slot does not.
// Legacy callers without a scope, and coherent-average requests, keep their
// original completion semantics. Scopes nest and never affect another thread.
class ScopedDecodeCancellation final {
public:
    ScopedDecodeCancellation(std::atomic<std::uint64_t> const& boundary,
                             std::uint64_t requestSerial, bool enabled)
        : previousBoundary(cancellation_detail::retired), previousSerial(cancellation_detail::serial)
    {
        cancellation_detail::retired = enabled ? &boundary : nullptr;
        cancellation_detail::serial = requestSerial;
    }
    ~ScopedDecodeCancellation()
    {
        cancellation_detail::retired = previousBoundary;
        cancellation_detail::serial = previousSerial;
    }
    ScopedDecodeCancellation(ScopedDecodeCancellation const&) = delete;
    ScopedDecodeCancellation& operator=(ScopedDecodeCancellation const&) = delete;
private:
    std::atomic<std::uint64_t> const* previousBoundary;
    std::uint64_t previousSerial;
};

inline bool decodeCancelled()
{
    auto const* boundary = cancellation_detail::retired;
    return boundary && cancellation_detail::serial != 0
        && boundary->load(std::memory_order_acquire) >= cancellation_detail::serial;
}
}}
#endif
