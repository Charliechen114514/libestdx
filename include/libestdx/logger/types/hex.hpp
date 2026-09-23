/**
 * @file hex.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Hex Wrapper Vocabulary
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <concepts>

namespace estdx::logger {

/**
 * @brief A hex wrapper: log::Hex(0x2a) appends "0x2a" (lowercase, no zero
 * padding; fixed-width hex is a later concern).
 *
 * bool is excluded on purpose: it satisfies std::integral, but to_chars has
 * no bool overload (deleted), so Hex<bool> would explode deep inside the
 * integer path.
 */
template <typename T>
    requires std::integral<T> && (!std::same_as<T, bool>)
struct Hex {
    using LogAsHex = void; // IsHex probe tag
    T value;
};

template <typename T>
    requires std::integral<T> && (!std::same_as<T, bool>)
Hex(T) -> Hex<T>;

template <typename T>
concept IsHex = requires { typename T::LogAsHex; };

} // namespace estdx::logger
