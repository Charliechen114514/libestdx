/**
 * @file format.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Formatter Layer
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

// How a type becomes log text. Shaped after std::formatter: specialize
// Formatter<T> with static format(LineAppender auto&, const T&); call sites
// only ever say line.append(value). No buffer state lives here — byte safety
// is buffer.hpp's job.

#include "libestdx/logger/buffer.hpp"
#include "libestdx/logger/types/hex.hpp"
#include "libestdx/logger/types/tag.hpp"

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <string_view>

namespace estdx::logger {

namespace detail {

// Integers via to_chars straight into the window. On failure to_chars writes
// nothing, so push the cursor to the cap and let commit's over-window branch
// flag the truncation. value_too_large can only mean the window ran out
// (longest integer text is int64_min, 20 chars).
template <std::integral T>
void write_integral(LineAppender auto& out, T v, int base) {
    const auto w = out.window();
    const auto r = std::to_chars(w.data(), w.data() + w.size(), v, base);
    out.commit(r.ec == std::errc{} ? static_cast<std::size_t>(r.ptr - w.data()) : w.size());
}

} // namespace detail

// Primary: unspecialized means unsupported; the message says how to fix it.
template <typename T>
struct Formatter {
    static void format(LineAppender auto&, const T&) {
        static_assert(!sizeof(T),
                      "logger: no formatter for T; specialize logger::Formatter<T> or wrap in Hex");
    }
};

// to_chars has no bool overload; spell it out.
template <>
struct Formatter<bool> {
    static void format(LineAppender auto& out, bool v) {
        out.append(std::string_view{v ? "true" : "false"});
    }
};

// Anything viewable as text: literals, const char*, string_view, std::string.
// The const-lvalue form mirrors the body's actual expression.
template <TextSource T>
struct Formatter<T> {
    static void format(LineAppender auto& out, const T& v) { out.append(std::string_view{v}); }
};

// Always "0x" + lowercase hex, no zero padding.
template <IsHex T>
struct Formatter<T> {
    static void format(LineAppender auto& out, const T& h) {
        out.append(std::string_view{"0x"});
        detail::write_integral(out, h.value, 16);
    }
};

// Integers (char/bool intercepted by their own specializations): to_chars, base 10.
template <typename T>
    requires std::integral<T> && (!std::same_as<T, char>) && (!std::same_as<T, bool>)
struct Formatter<T> {
    static void format(LineAppender auto& out, T v) { detail::write_integral(out, v, 10); }
};

// disabled char only, which, easy convert to intergrals
template <>
struct Formatter<char> {
    static void format(LineAppender auto& out, char) {
        static_assert(sizeof(out) == 0,
                      "logger: single chars are not in the logging vocabulary; pass a string_view");
    }
};

// FP to_chars drags formatting tables into flash; rejected.
template <typename T>
    requires std::floating_point<T>
struct Formatter<T> {
    static void format(LineAppender auto&, const T&) {
        static_assert(
            !sizeof(T),
            "logger: float rejected (FP formatting tables cost flash); use Hex or fixed-point");
    }
};

} // namespace estdx::logger
