/**
 * @file logger.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief Logger Core
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "libestdx/logger/base/format_string.hpp"
#include "libestdx/logger/buffer.hpp"
#include "libestdx/logger/format.hpp"
#include "libestdx/logger/types/core.hpp"
#include "libestdx/logger/types/level.hpp"
#include "libestdx/logger/types/tag.hpp"

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace estdx::logger {

// kMinimumLevel is the floor compiled into the firmware: below it the
// wrappers' bodies are empty, and at -O3 + function/data-sections +
// --gc-sections even their string literals vanish. Runtime filtering can
// only narrow or widen within that floor.
// Stack budget: each active log call holds one transient frame of roughly
// kLineBytes plus inlining scratch (measured 376B at 128, M3 -O3).
template <LogSink ASink, LogLevel kMinimumLevel = LogLevel::Info, std::size_t kLineBytes = 128,
          LogClockOrNone AClock = NoClock, bool kLocation = false>
struct Logger {
    // Line protocol: presentation policy lives with its only user.
    static constexpr char kTruncateMark = '~';
    static constexpr std::string_view kLineEnd = "\r\n";

    // The single instance. Constant-initialized member, so no guard variable
    // even without -fno-threadsafe-statics.
    static Logger& Self() noexcept {
        static Logger ALogger;
        return ALogger;
    }

    Logger& setLevel(LogLevel l) {
        runtime_level_ = l;
        return *this;
    }
    LogLevel level() const noexcept { return runtime_level_; }

    // The one mechanism entry: explicit compile-time level.
    template <LogLevel kRequestLevel, typename... Parts>
    void log(Tag tag, Parts&&... parts) {
        if constexpr (kRequestLevel >= kMinimumLevel) {
            emit<kRequestLevel>(tag, std::forward<Parts>(parts)...);
        }
    }

    // Formatted flavor: format_to syntax for the payload, same pipeline.
    template <LogLevel kRequestLevel, typename... Parts>
    void logf(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt, Parts&&... parts) {
        if constexpr (kRequestLevel >= kMinimumLevel) {
            emit_f<kRequestLevel>(tag, fmt, std::forward<Parts>(parts)...);
        }
    }

  private:
    // Per-segment format_to (NOT one big format string): the clock/location
    // combinations stay independent segments instead of a 4-way permutation.
    template <LogLevel kUseLevel>
    static void append_header(LineBuffer<kLineBytes>& line, const Tag& tag) {
        if constexpr (LogClock<AClock>) {  // NoClock: segment not even instantiated
            format_to(line, "[{}ms]", AClock::now_ms());
        }
        format_to(line, "[{:5}]", LevelName(kUseLevel));
        format_to(line, "[{}]", tag.text);
        if constexpr (kLocation) {
            format_to(line, "[{}:{}]", tag.location.file_name(), tag.location.line());
        }
        line.append(" ");
    }

    template <LogLevel kUseLevel, typename... Parts>
    void emit(Tag tag, Parts&&... parts) {
        if (kUseLevel < runtime_level_) {
            return;  // compiled in, silenced at runtime
        }
        LineBuffer<kLineBytes> line;
        append_header<kUseLevel>(line, tag);
        (line.append(std::forward<Parts>(parts)), ...);  // fold: order = argument order
        ASink::write(line.finish(kTruncateMark, kLineEnd));
    }

    template <LogLevel kUseLevel, typename... Parts>
    void emit_f(Tag tag, FormatString<std::type_identity_t<Parts>...> fmt, Parts&&... parts) {
        if (kUseLevel < runtime_level_) {
            return;
        }
        LineBuffer<kLineBytes> line;
        append_header<kUseLevel>(line, tag);
        format_to(line, fmt, std::forward<Parts>(parts)...);
        ASink::write(line.finish(kTruncateMark, kLineEnd));
    }

    LogLevel runtime_level_{kMinimumLevel};
};

} // namespace estdx::logger
