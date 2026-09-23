#pragma once
#include <concepts>
#include <cstdint>
#include <span>

namespace estdx::logger {

template <typename ALogSink>
concept LogSink = requires(std::span<const char> aLine) {
    { ALogSink::write(aLine) } -> std::same_as<void>;
};

template <typename AClockable>
concept LogClock = requires {
    // Clockable Tells us the current time
    { AClockable::now_ms() } -> std::convertible_to<std::uint32_t>;
};

struct NoClock {};

/**
 * @brief Some MCU is hard to tell the clock time, and that is it
 *
 * @tparam T
 */
template <typename T>
concept LogClockOrNone = std::same_as<T, NoClock> || LogClock<T>;

} // namespace estdx::logger