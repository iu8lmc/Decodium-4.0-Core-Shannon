#ifndef DECODIUM_FST4_DECODE_CANCELLATION_HPP
#define DECODIUM_FST4_DECODE_CANCELLATION_HPP

#include <atomic>
#include <cstdint>

namespace decodium { namespace fst4 {
namespace cancellation_detail {
inline thread_local std::atomic<std::uint64_t> const* retired = nullptr;
inline thread_local std::uint64_t serial = 0;
inline thread_local std::atomic_bool const* shutdown = nullptr;
}

// A newer slot does not cancel work. FST4W ignores session retirement but
// can still stop when the application shuts down. Scopes are thread-local.
class ScopedDecodeCancellation final {
public:
    ScopedDecodeCancellation(std::atomic<std::uint64_t> const& boundary,
                             std::uint64_t requestSerial, bool enabled,
                             std::atomic_bool const* shutdown = nullptr)
        : previousBoundary(cancellation_detail::retired), previousSerial(cancellation_detail::serial),
          previousShutdown(cancellation_detail::shutdown)
    {
        cancellation_detail::retired = enabled ? &boundary : nullptr;
        cancellation_detail::serial = requestSerial;
        cancellation_detail::shutdown = shutdown;
    }
    ~ScopedDecodeCancellation()
    {
        cancellation_detail::retired = previousBoundary;
        cancellation_detail::serial = previousSerial;
        cancellation_detail::shutdown = previousShutdown;
    }
    ScopedDecodeCancellation(ScopedDecodeCancellation const&) = delete;
    ScopedDecodeCancellation& operator=(ScopedDecodeCancellation const&) = delete;
private:
    std::atomic<std::uint64_t> const* previousBoundary;
    std::uint64_t previousSerial;
    std::atomic_bool const* previousShutdown;
};

inline bool decodeCancelled()
{
    auto const* shutdown = cancellation_detail::shutdown;
    if (shutdown && shutdown->load(std::memory_order_acquire)) return true;
    auto const* boundary = cancellation_detail::retired;
    return boundary && cancellation_detail::serial != 0
        && boundary->load(std::memory_order_acquire) >= cancellation_detail::serial;
}
}}
#endif
