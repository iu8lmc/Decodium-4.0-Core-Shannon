#ifndef DECODIUM_FT4_DECODE_CANCELLATION_HPP
#define DECODIUM_FT4_DECODE_CANCELLATION_HPP

#include <atomic>
#include <cstdint>

namespace decodium { namespace ft4 {

// The owning worker increments the generation only when leaving the mode or
// shutting down. New slots in the same mode must not cancel an active pass.
// The scope is thread-local; independent decoder callers retain their context.
class ScopedDecodeCancellation final
{
public:
    ScopedDecodeCancellation(std::atomic<std::uint64_t> const& generation,
                             std::uint64_t expected);
    ~ScopedDecodeCancellation();
    ScopedDecodeCancellation(ScopedDecodeCancellation const&) = delete;
    ScopedDecodeCancellation& operator=(ScopedDecodeCancellation const&) = delete;
private:
    std::atomic<std::uint64_t> const* m_previousGeneration;
    std::uint64_t m_previousExpected;
};

}}
#endif
