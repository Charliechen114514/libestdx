#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace estdx::uart {

// A completed call has transferred the entire span. Implementations must not
// return normally after a failed or partial transfer.
template <typename T>
concept UartWriter = requires(std::span<const std::byte> data) {
    { T::send(data) } -> std::same_as<void>;
};

template <typename T>
concept UartReader = requires(std::span<std::byte> buffer) {
    { T::receive(buffer) } -> std::same_as<void>;
};

// Nine data bits require one uint16_t per UART symbol.
template <typename T>
concept UartWordWriter = requires(std::span<const std::uint16_t> data) {
    { T::send(data) } -> std::same_as<void>;
};

template <typename T>
concept UartWordReader = requires(std::span<std::uint16_t> buffer) {
    { T::receive(buffer) } -> std::same_as<void>;
};

} // namespace estdx::uart
