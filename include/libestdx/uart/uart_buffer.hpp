#pragma once
#include <array>
#include <cstddef>
#include <span>

namespace estdx::uart {
template <std::size_t BufferSize>
using UartBufferContainer = std::array<std::byte, BufferSize>;

using UartBufferView = std::span<std::byte>;
using UartConstBufferView = std::span<const std::byte>;

template <std::size_t TakeBytesCount, std::size_t BufferSize>
    requires(TakeBytesCount <= BufferSize)
constexpr auto MakeView(UartBufferContainer<BufferSize>& buffer) {
    return std::span<std::byte, TakeBytesCount>{buffer.data(), TakeBytesCount};
}

template <std::size_t TakeBytesCount, std::size_t BufferSize>
    requires(TakeBytesCount <= BufferSize)
constexpr auto MakeView(const UartBufferContainer<BufferSize>& buffer) {
    return std::span<const std::byte, TakeBytesCount>{buffer.data(), TakeBytesCount};
}

template <std::size_t BufferSize>
constexpr auto MakeView(UartBufferContainer<BufferSize>& buffer) {
    return std::span<std::byte, BufferSize>{buffer};
}

template <std::size_t BufferSize>
constexpr auto MakeView(const UartBufferContainer<BufferSize>& buffer) {
    return std::span<const std::byte, BufferSize>{buffer};
}

} // namespace estdx::uart
