/**
 * @file format_string.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Format-string Syntax On Top Of The Formatter Layer
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

// A minimal std::format-like syntax riding on the Formatter layer:
//   format_to(out, "count={} hex={:x}", n, v)
// Supported: "{}" (any formattable value), "{:x}" (integral, via Hex),
// "{{" and "}}" escapes. Everything is checked at compile time P2216-style:
// field count, syntax, spec vocabulary. Compile-time rejection uses
// non-constexpr beacon functions — NOT consteval throw, which -fno-exceptions
// rejects syntactically even on never-taken branches.

#include "libestdx/logger/format.hpp"
#include "libestdx/logger/types/hex.hpp"
#include "libestdx/logger/types/tag.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace estdx::logger {

namespace detail {

// Failure beacons: intentionally non-constexpr. A consteval evaluation that
// reaches such a call is rejected; the function's name is the reason.
int format_string_field_count_mismatch();
int format_string_unterminated_field();
int format_string_unknown_spec();
int format_string_x_requires_integral();

// Field content grammar: "" | ":x" | ":<digits>". Anything else — arg-ids,
// junk — is unsupported.
template <typename... Args>
consteval void validate_format_string(std::string_view s) {
    // Hex constructibility is the single source of truth for ":x" legality.
    constexpr std::array<bool, sizeof...(Args)> kHexable{(requires { Hex{std::declval<Args>()}; })...};
    std::size_t fields = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '{') {
            continue;
        }
        if (i + 1 < s.size() && s[i + 1] == '{') {
            ++i;  // "{{" escape
            continue;
        }
        const std::size_t close = s.find('}', i);
        if (close == std::string_view::npos) {
            format_string_unterminated_field();
        }
        const std::string_view content = s.substr(i + 1, close - i - 1);
        if (!content.empty()) {
            if (content.front() != ':') {
                format_string_unknown_spec();
            }
            const std::string_view spec = content.substr(1);
            if (spec == "x") {
                // Per-field type check: the runtime x-branch compiles for
                // EVERY frame, so a non-hexable head must be rejected here.
                if (!kHexable[fields]) {
                    format_string_x_requires_integral();
                }
            } else {
                for (const char c : spec) {
                    if (c < '0' || c > '9') {
                        format_string_unknown_spec();
                    }
                }
            }
        }
        ++fields;
        i = close;
    }
    if (fields != sizeof...(Args)) {
        format_string_field_count_mismatch();
    }
}

// Literal chunk with "{{" and "}}" each collapsed to a single brace. Chunks
// between fields contain no '{' by construction (the scanner stops before
// one); the no-argument tail path can still carry both escapes.
inline void append_escaped(LineAppender auto& out, std::string_view chunk) {
    std::size_t start = 0;
    for (std::size_t i = 0; i + 1 < chunk.size(); ++i) {
        const bool doubled = (chunk[i] == '{' && chunk[i + 1] == '{') ||
                              (chunk[i] == '}' && chunk[i + 1] == '}');
        if (doubled) {
            out.append(chunk.substr(start, i + 1 - start));  // one copy of the brace
            start = i + 2;
            ++i;
        }
    }
    out.append(chunk.substr(start));
}

// Order-recursive rendering: literal chunks go straight out, each field
// consumes the head argument, recursion takes the rest. No tuple, no index
// lookup. Specs were validated at compile time; only "" and "x" reach here.
inline void render(LineAppender auto& out, std::string_view fmt, std::size_t pos) {
    append_escaped(out, fmt.substr(pos));  // tail: no fields left
}

inline void render(LineAppender auto& out, std::string_view fmt, std::size_t pos, auto&& head,
                   auto&&... rest) {
    const std::size_t open = fmt.find('{', pos);
    if (open == std::string_view::npos) {
        append_escaped(out, fmt.substr(pos));
        return;
    }
    append_escaped(out, fmt.substr(pos, open - pos));
    if (open + 1 < fmt.size() && fmt[open + 1] == '{') {  // "{{" escape
        out.append(std::string_view{"{"});
        render(out, fmt, open + 2, std::forward<decltype(head)>(head),
               std::forward<decltype(rest)>(rest)...);
        return;
    }
    const std::size_t close = fmt.find('}', open);  // found: validated at compile time
    const std::string_view content = fmt.substr(open + 1, close - open - 1);  // "" or ":spec"
    if (content == ":x") {
        // The guard keeps the x-branch instantiable for ANY head: this if is
        // runtime, so the branch is compiled for every frame. For validated
        // strings the else can never run — a non-hexable head was already
        // rejected at compile time by the per-field check.
        if constexpr (requires { Hex{std::forward<decltype(head)>(head)}; }) {
            out.append(Hex{std::forward<decltype(head)>(head)});
        } else {
            out.append(std::forward<decltype(head)>(head));
        }
    } else {
        // "" or ":<digits>": default rendering, then left-pad to width.
        // Pad-only (std::format width semantics) — never truncates.
        std::size_t width = 0;
        for (const char c : content.substr(content.empty() ? 0 : 1)) {
            width = width * 10 + static_cast<std::size_t>(c - '0');
        }
        const std::size_t before = out.size();
        out.append(std::forward<decltype(head)>(head));
        const std::size_t used = out.size() - before;
        if (width > used) {
            out.fill(' ', width - used);
        }
    }
    render(out, fmt, close + 1, std::forward<decltype(rest)>(rest)...);
}

} // namespace detail

// The Args pack exists so the consteval constructor can check the field
// count against the argument count — the P2216 guarantee.
template <typename... Args>
struct FormatString {
    std::string_view text;

    template <TextSource S>
    consteval FormatString(S&& s) : text(std::forward<S>(s)) {
        detail::validate_format_string<Args...>(text);
    }
};

// The only entry: the same Formatter dispatch as concatenation runs under
// this syntax — one substrate, two syntaxes. type_identity_t keeps Args
// non-deduced here (the std::format_string trick): Args is deduced by the
// trailing pack only, then the literal converts via the consteval ctor —
// otherwise the two deductions fight and a raw "..." never matches.
template <typename... Args>
void format_to(LineAppender auto& out, FormatString<std::type_identity_t<Args>...> fmt,
               Args&&... args) {
    detail::render(out, fmt.text, 0, std::forward<Args>(args)...);
}

} // namespace estdx::logger
