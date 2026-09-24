#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

namespace estdx::base {

// Single-producer single-consumer queue, no locks, no atomics.
//
// Concurrency contract: exactly ONE context calls push() (typically an ISR)
// and exactly ONE other context calls pop() (typically the main loop).
// Adding a second producer or consumer voids the contract — that needs
// critical sections, not this class.
//
// Why plain volatile indices suffice on a single-core Cortex-M: each index
// has exactly one writer, aligned 32-bit loads/stores are single bus
// accesses (no tearing), and a reader observing a stale index merely
// revisits "empty"/"full" — it can never see a half-written element, because
// the writer publishes the index AFTER storing the element. volatile only
// stops the compiler from caching indices in registers across the ISR
// boundary; it is not a memory barrier and not needed as one here (no data
// cache on the M3, and code ordering within push/pop is fixed by the
// data dependency).
//
// Why one slot stays unused: head == tail unambiguously means empty, and
// full is detected one slot early. The classic alternative — an extra count
// variable — buys back the slot but pays with a second shared variable that
// both sides must reason about. One spare byte of RAM is the cheaper trade.
template <typename T, std::size_t N>
class SpscQueue {
    static_assert(N > 0 && (N & (N - 1)) == 0, "N must be a power of two (mask-based wrap)");
    static_assert(std::is_trivially_copyable_v<T>, "elements cross an ISR boundary by plain copy");

public:
    // Producer side (ISR). Returns false when full; the caller owns the
    // overflow policy (drop, count, overwrite — but never block in an ISR).
    bool push(const T& value) {
        const auto head = head_;
        const auto next = wrap(head + 1);
        if (next == tail_) {
            return false;
        }
        storage_[head] = value;
        head_ = next;  // element first, index last — the release order readers rely on
        return true;
    }

    // Consumer side (main loop). Returns false when empty.
    bool pop(T& out) {
        const auto tail = tail_;
        if (tail == head_) {
            return false;
        }
        out = storage_[tail];
        tail_ = wrap(tail + 1);
        return true;
    }

    bool empty() const { return head_ == tail_; }

    // Approximate occupancy: the other side's index may move concurrently,
    // so treat this as a hint (for buffer sizing), not a fact.
    std::size_t available() const { return (head_ - tail_) & (N - 1); }

    static constexpr std::size_t capacity() { return N - 1; }

private:
    static constexpr std::size_t wrap(std::size_t index) { return index & (N - 1); }

    std::array<T, N> storage_{};
    volatile std::size_t head_ = 0;  // written only by the producer
    volatile std::size_t tail_ = 0;  // written only by the consumer
};

} // namespace estdx::base
