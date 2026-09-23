/**
 * @file buffer.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Buffer Abstrations
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "libestdx/logger/config.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

namespace estdx::logger {

// If a buffer wants to be a logger line, it must offer these operations.
// Constraining `out` here fails at the signature instead of deep inside a
// function body.
template <typename T>
concept LineAppender = requires(T t, char c, std::string_view sv, std::size_t n) {
    t.append(sv);                                           // text bytes
    t.fill(c, n);                                           // repeated byte fill
    { t.window() } -> std::convertible_to<std::span<char>>; // to_chars window
    t.commit(n);                                            // advance after window write
    { t.size() } -> std::convertible_to<std::size_t>;       // width specs measure through this
};

// Defined in format.hpp; append(T) only needs it at the instantiation point.
template <typename T>
struct Formatter;

/**
 * @brief A stack line assembler: append parts, finish() yields the whole
 * line for one single Sink::write.
 *
 * Invariant pos_ <= BufferSize - 3: content lives in [0, kContentBufferLength),
 * the last 3 bytes are reserved forever for finish()'s truncation mark + line
 * end, so no append ever needs to shift bytes to make room. buf_ is left
 * uninitialized: only [0, pos_) is ever exposed.
 */
template <std::size_t BufferSize>
struct LineBuffer {
    static constexpr auto kContentBufferLength = BufferSize - 3;
    static_assert(BufferSize >= kLineBufferSize && BufferSize > 3,
                  "line buffer must be at least kLineBufferSize");

    // ---- Byte primitives: one owner per action, no presentation choice ----

    // The only path text bytes enter.
    void append(std::string_view view) {
        const std::size_t take = std::min(view.size(), kContentBufferLength - pos_);
        std::copy_n(view.data(), take, buf_.data() + pos_);
        pos_ += take;
        if (take < view.size()) {
            truncated_ = true;
        }
    }

    // The only path for repeated-byte fill; the byte itself is caller's choice.
    void fill(char c, std::size_t count) {
        const std::size_t fills = std::min(count, kContentBufferLength - pos_);
        std::fill_n(buf_.data() + pos_, fills, c);
        pos_ += fills;
        if (fills < count) {
            truncated_ = true;
        }
    }

    // Reserved for direct-writing serializers (the to_chars family);
    // do not memcpy text through it.
    constexpr std::span<char> window() noexcept {
        return {buf_.data() + pos_, kContentBufferLength - pos_};
    }

    void commit(std::size_t written) {
        const std::size_t room = kContentBufferLength - pos_;
        if (written <= room) {
            pos_ += written;
        } else {
            pos_ = kContentBufferLength;
            truncated_ = true;
        }
    }

    // ---- Caller entry points ----

    // The only typed entry: regular code touches this and finish() only.
    template <typename T>
    void append(const T& value) {
        Formatter<std::remove_cvref_t<T>>::format(*this, value);
    }

    std::size_t size() const { return pos_; }

    // Seal the line. Budget: 1 truncate mark + line end of at most 2 bytes.
    // The mechanism (reserved tail) lives here; the characters are caller's
    // choice. Call at most once — a second call writes out of bounds (UB).
    std::span<const char> finish(char truncate_mark, std::string_view line_end) {
        if (truncated_) {
            put_raw(truncate_mark);
        }
        for (char c : line_end) {
            if (pos_ >= BufferSize) {
                break; // line end over budget: clamp, never write past the buffer
            }
            put_raw(c);
        }
        return {buf_.data(), pos_};
    }

  private:
    // Unclamped write for the reserved tail only; caller keeps pos_ < BufferSize.
    void put_raw(char c) { buf_[pos_++] = c; }

    std::array<char, BufferSize> buf_;
    std::size_t pos_ = 0;
    bool truncated_ = false;
};

} // namespace estdx::logger
